#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

static_assert(sizeof(KernelTimezone) == 8);
static_assert(alignof(KernelTimezone) == 4);
static_assert(offsetof(KernelTimezone, tz_minuteswest) == 0);
static_assert(offsetof(KernelTimezone, tz_dsttime) == 4);

static_assert(sizeof(KernelTimesec) == 16);
static_assert(alignof(KernelTimesec) == 8);
static_assert(offsetof(KernelTimesec, t) == 0);
static_assert(offsetof(KernelTimesec, west_sec) == 8);
static_assert(offsetof(KernelTimesec, dst_sec) == 12);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestTimezoneLayoutAndDefaults() {
    KernelTimezone tz{-9999, -9999};
    Require(sceKernelGettimezone(&tz) == 0);
    Require(tz.tz_minuteswest >= -720 && tz.tz_minuteswest <= 840);
    Require(tz.tz_dsttime >= 0 && tz.tz_dsttime <= 120);
}

void TestTimezoneNullAndFault() {
    Require(sceKernelGettimezone(nullptr) == SCE_KERNEL_ERROR_EFAULT);

    auto* const badTimezone = reinterpret_cast<KernelTimezone*>(static_cast<std::uintptr_t>(0x10));
    Require(sceKernelGettimezone(badTimezone) == SCE_KERNEL_ERROR_EFAULT);

    auto* const unalignedBadTimezone = reinterpret_cast<KernelTimezone*>(static_cast<std::uintptr_t>(0x1));
    Require(sceKernelGettimezone(unalignedBadTimezone) == SCE_KERNEL_ERROR_EFAULT);
}

void TestConvertNullOutputs() {
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, nullptr, nullptr) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, nullptr, nullptr) == 0);

    int64_t timeOut = 0;
    KernelTimesec stateOut{};
    int32_t dstOut = 0;

    Require(sceKernelConvertLocaltimeToUtc(0, 0, &timeOut, nullptr, nullptr) == 0);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, &stateOut, nullptr) == 0);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, nullptr, &dstOut) == 0);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, &timeOut, &stateOut, nullptr) == 0);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, &timeOut, nullptr, &dstOut) == 0);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, &stateOut, &dstOut) == 0);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, &timeOut, &stateOut, &dstOut) == 0);

    Require(sceKernelConvertUtcToLocaltime(0, &timeOut, nullptr, nullptr) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, &stateOut, nullptr) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, nullptr, &dstOut) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, &timeOut, &stateOut, nullptr) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, &timeOut, nullptr, &dstOut) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, &stateOut, &dstOut) == 0);
    Require(sceKernelConvertUtcToLocaltime(0, &timeOut, &stateOut, &dstOut) == 0);
}

void TestConvertReservedParameter() {
    int64_t utc0 = 0;
    int64_t utc1 = 0;
    int64_t utcNeg = 0;
    int64_t utcLarge = 0;

    Require(sceKernelConvertLocaltimeToUtc(1000, 0, &utc0, nullptr, nullptr) == 0);
    Require(sceKernelConvertLocaltimeToUtc(1000, 1, &utc1, nullptr, nullptr) == 0);
    Require(sceKernelConvertLocaltimeToUtc(1000, -1, &utcNeg, nullptr, nullptr) == 0);
    Require(sceKernelConvertLocaltimeToUtc(1000, 99999, &utcLarge, nullptr, nullptr) == 0);

    Require(utc0 == utc1);
    Require(utc0 == utcNeg);
    Require(utc0 == utcLarge);
}

void TestBadPointerReturnsEfault() {
    auto* const badTime = reinterpret_cast<int64_t*>(static_cast<std::uintptr_t>(0x10));
    auto* const badState = reinterpret_cast<KernelTimesec*>(static_cast<std::uintptr_t>(0x10));
    auto* const badDst = reinterpret_cast<int32_t*>(static_cast<std::uintptr_t>(0x10));

    Require(sceKernelConvertLocaltimeToUtc(0, 0, badTime, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, badState, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelConvertLocaltimeToUtc(0, 0, nullptr, nullptr, badDst) == SCE_KERNEL_ERROR_EFAULT);

    Require(sceKernelConvertUtcToLocaltime(0, badTime, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, badState, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelConvertUtcToLocaltime(0, nullptr, nullptr, badDst) == SCE_KERNEL_ERROR_EFAULT);
}

void TestOutputWriteOrderOnPartialFault() {
    auto* const badTime = reinterpret_cast<int64_t*>(static_cast<std::uintptr_t>(0x10));
    auto* const badState = reinterpret_cast<KernelTimesec*>(static_cast<std::uintptr_t>(0x10));
    auto* const badDst = reinterpret_cast<int32_t*>(static_cast<std::uintptr_t>(0x10));

    int64_t utc = -12345;
    Require(sceKernelConvertLocaltimeToUtc(0, 0, &utc, badState, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(utc != -12345);

    utc = -12345;
    KernelTimesec localState{-12345, 0xdeadbeefu, 0xdeadbeefu};
    Require(sceKernelConvertLocaltimeToUtc(0, 0, &utc, &localState, badDst) == SCE_KERNEL_ERROR_EFAULT);
    Require(utc != -12345);
    Require(localState.t == 0);
    Require(localState.west_sec != 0xdeadbeefu);

    KernelTimesec untouchedState{-12345, 0xdeadbeefu, 0xdeadbeefu};
    int32_t untouchedDst = -12345;
    Require(sceKernelConvertLocaltimeToUtc(0, 0, badTime, &untouchedState, &untouchedDst) == SCE_KERNEL_ERROR_EFAULT);
    Require(untouchedState.t == -12345);
    Require(untouchedState.west_sec == 0xdeadbeefu);
    Require(untouchedDst == -12345);

    int64_t local = -12345;
    Require(sceKernelConvertUtcToLocaltime(0, &local, badState, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(local != -12345);

    local = -12345;
    KernelTimesec utcState{-12345, 0xdeadbeefu, 0xdeadbeefu};
    Require(sceKernelConvertUtcToLocaltime(0, &local, &utcState, badDst) == SCE_KERNEL_ERROR_EFAULT);
    Require(local != -12345);
    Require(utcState.t == 0);
    Require(utcState.west_sec != 0xdeadbeefu);

    untouchedState = {-12345, 0xdeadbeefu, 0xdeadbeefu};
    untouchedDst = -12345;
    Require(sceKernelConvertUtcToLocaltime(0, badTime, &untouchedState, &untouchedDst) == SCE_KERNEL_ERROR_EFAULT);
    Require(untouchedState.t == -12345);
    Require(untouchedState.west_sec == 0xdeadbeefu);
    Require(untouchedDst == -12345);
}

void TestTimezoneMinutesWestAndDstSecConsistency() {
    KernelTimezone tz{-9999, -9999};
    Require(sceKernelGettimezone(&tz) == 0);

    constexpr std::array<int64_t, 5> timestamps{{
        -86400, 0, 1000000, 1768478400, 2147483647
    }};

    for (const auto utcIn : timestamps) {
        int64_t localTime = -1;
        KernelTimesec utcState{-1, 0, 0};
        int32_t dstSec = -1;

        Require(sceKernelConvertUtcToLocaltime(utcIn, &localTime, &utcState, &dstSec) == 0);
        Require(utcState.t == utcIn);
        Require(static_cast<int32_t>(utcState.west_sec) == tz.tz_minuteswest * 60);
        Require(utcState.dst_sec == static_cast<uint32_t>(dstSec));
        Require(dstSec >= 0);
        Require(dstSec <= tz.tz_dsttime * 60);
        Require(localTime == utcIn + static_cast<int32_t>(utcState.west_sec) + dstSec);

        int64_t utcOut = -1;
        KernelTimesec localState{-1, 0, 0};
        int32_t dstAmount = -1;

        Require(sceKernelConvertLocaltimeToUtc(localTime, 0, &utcOut, &localState, &dstAmount) == 0);
        Require(localState.t == localTime);
        Require(localState.west_sec == utcState.west_sec);
        Require(dstAmount == tz.tz_dsttime * 60);
        Require(utcOut == utcIn);
    }
}

} // namespace

int main() {
    TestTimezoneLayoutAndDefaults();
    TestTimezoneNullAndFault();
    TestConvertNullOutputs();
    TestConvertReservedParameter();
    TestBadPointerReturnsEfault();
    TestOutputWriteOrderOnPartialFault();
    TestTimezoneMinutesWestAndDstSecConsistency();
    return 0;
}
