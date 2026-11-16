#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

int SetTimeOfDay_nid_no_patch(const KernelTimeval* tv, const KernelTimezone* tz);

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EACCES = 13;

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

static void TestGettimeofdayNullAndValidHandling() {
    Require(gettimeofday_nid_postfix(nullptr, nullptr) == SCE_OK);

    KernelTimeval tv{};
    Require(gettimeofday_nid_postfix(&tv, nullptr) == SCE_OK);
    Require(tv.tv_sec > 0);
    Require(tv.tv_usec >= 0 && tv.tv_usec < 1000000);

    KernelTimezone tz{-1, -1};
    Require(gettimeofday_nid_postfix(nullptr, &tz) == SCE_OK);
    Require(tz.tz_minuteswest == 0 && tz.tz_dsttime == 0);

    tv = {};
    tz = {-1, -1};
    Require(gettimeofday_nid_postfix(&tv, &tz) == SCE_OK);
    Require(tv.tv_sec > 0);
    Require(tv.tv_usec >= 0 && tv.tv_usec < 1000000);
    Require(tz.tz_minuteswest == 0 && tz.tz_dsttime == 0);

    KernelTimeval tv2{};
    Require(gettimeofday_nid_postfix(&tv2, nullptr) == SCE_OK);
    Require(tv2.tv_sec > tv.tv_sec || (tv2.tv_sec == tv.tv_sec && tv2.tv_usec >= tv.tv_usec));
}

static void TestGettimeofdayFaultHandling() {
    KernelTimeval validTv{};
    KernelTimezone validTz{};

    for (const std::uintptr_t badAddress : {std::uintptr_t{1}, std::uintptr_t{0x10}}) {
        auto* badTv = reinterpret_cast<KernelTimeval*>(badAddress);
        auto* badTz = reinterpret_cast<KernelTimezone*>(badAddress);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(gettimeofday_nid_postfix(badTv, nullptr) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(gettimeofday_nid_postfix(nullptr, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(gettimeofday_nid_postfix(badTv, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(gettimeofday_nid_postfix(&validTv, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(gettimeofday_nid_postfix(badTv, &validTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);
    }
}

static void TestSceGettimeofdayAndZone() {
    Require(sceKernelGettimeofday(nullptr) == SCE_KERNEL_ERROR_EFAULT);

    for (const std::uintptr_t badAddress : {std::uintptr_t{1}, std::uintptr_t{0x10}}) {
        auto* badTv = reinterpret_cast<KernelTimeval*>(badAddress);
        Require(sceKernelGettimeofday(badTv) == SCE_KERNEL_ERROR_EFAULT);
    }

    KernelTimeval tv{};
    Require(sceKernelGettimeofday(&tv) == SCE_OK);
    Require(tv.tv_sec > 0);
    Require(tv.tv_usec >= 0 && tv.tv_usec < 1000000);

    Require(sceKernelGettimezone(nullptr) == SCE_KERNEL_ERROR_EFAULT);

    for (const std::uintptr_t badAddress : {std::uintptr_t{1}, std::uintptr_t{0x10}}) {
        auto* badTz = reinterpret_cast<KernelTimezone*>(badAddress);
        Require(sceKernelGettimezone(badTz) == SCE_KERNEL_ERROR_EFAULT);
    }

    KernelTimezone tz{-1, -1};
    Require(sceKernelGettimezone(&tz) == SCE_OK);
}

static void TestSettimeofdayParameterValidationAndNegativeUsecBounds() {
    const KernelTimezone validZone{60, 1};
    const KernelTimeval validTv{1000000000LL, 0};

    *__error_nid_postfix() = GUEST_EACCES;
    Require(settimeofday_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(SetTimeOfDay_nid_no_patch(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    Require(sceKernelSettimeofday(nullptr, nullptr) == SCE_KERNEL_ERROR_EPERM);

    const KernelTimeval invalidTimes[] = {
        {0, -1},
        {0, -500},
        {0, -1000000LL},
        {INT64_MIN, -1},
        {-1, -1},
        {-10, -500},
        {0, 1000000LL},
        {0, 2000000LL},
        {1000, 1000000LL},
        {-1, 0}
    };

    for (const std::uintptr_t badAddress : {std::uintptr_t{1}, std::uintptr_t{0x10}}) {
        const auto* badTv = reinterpret_cast<const KernelTimeval*>(badAddress);
        const auto* badTz = reinterpret_cast<const KernelTimezone*>(badAddress);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(settimeofday_nid_postfix(badTv, &validZone) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(settimeofday_nid_postfix(badTv, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(settimeofday_nid_postfix(nullptr, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(settimeofday_nid_postfix(&validTv, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(SetTimeOfDay_nid_no_patch(badTv, &validZone) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(SetTimeOfDay_nid_no_patch(nullptr, badTz) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        Require(sceKernelSettimeofday(badTv, &validZone) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelSettimeofday(nullptr, badTz) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelSettimeofday(badTv, badTz) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelSettimeofday(&validTv, badTz) == SCE_KERNEL_ERROR_EFAULT);

        for (const auto& invalid : invalidTimes) {
            *__error_nid_postfix() = GUEST_EACCES;
            Require(settimeofday_nid_postfix(&invalid, badTz) == -1);
            Require(*__error_nid_postfix() == GUEST_EFAULT);

            *__error_nid_postfix() = GUEST_EACCES;
            Require(SetTimeOfDay_nid_no_patch(&invalid, badTz) == -1);
            Require(*__error_nid_postfix() == GUEST_EFAULT);

            Require(sceKernelSettimeofday(&invalid, badTz) == SCE_KERNEL_ERROR_EFAULT);
        }
    }

    for (const auto& invalid : invalidTimes) {
        *__error_nid_postfix() = GUEST_EACCES;
        Require(settimeofday_nid_postfix(&invalid, &validZone) == -1);
        Require(*__error_nid_postfix() == GUEST_EPERM);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(settimeofday_nid_postfix(&invalid, nullptr) == -1);
        Require(*__error_nid_postfix() == GUEST_EPERM);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(SetTimeOfDay_nid_no_patch(&invalid, &validZone) == -1);
        Require(*__error_nid_postfix() == GUEST_EPERM);

        *__error_nid_postfix() = GUEST_EACCES;
        Require(SetTimeOfDay_nid_no_patch(&invalid, nullptr) == -1);
        Require(*__error_nid_postfix() == GUEST_EPERM);

        Require(sceKernelSettimeofday(&invalid, &validZone) == SCE_KERNEL_ERROR_EPERM);
        Require(sceKernelSettimeofday(&invalid, nullptr) == SCE_KERNEL_ERROR_EPERM);
    }

    *__error_nid_postfix() = GUEST_EACCES;
    Require(settimeofday_nid_postfix(&validTv, &validZone) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(settimeofday_nid_postfix(&validTv, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(settimeofday_nid_postfix(nullptr, &validZone) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(SetTimeOfDay_nid_no_patch(&validTv, &validZone) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    *__error_nid_postfix() = GUEST_EACCES;
    Require(SetTimeOfDay_nid_no_patch(&validTv, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    Require(sceKernelSettimeofday(&validTv, &validZone) == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelSettimeofday(&validTv, nullptr) == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelSettimeofday(nullptr, &validZone) == SCE_KERNEL_ERROR_EPERM);

    KernelTimeval current{};
    Require(gettimeofday_nid_postfix(&current, nullptr) == SCE_OK);
    Require(current.tv_sec != validTv.tv_sec);
}

int main() {
    TestGettimeofdayNullAndValidHandling();
    TestGettimeofdayFaultHandling();
    TestSceGettimeofdayAndZone();
    TestSettimeofdayParameterValidationAndNegativeUsecBounds();
    return 0;
}
