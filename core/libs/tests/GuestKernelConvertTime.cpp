#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static void TestNullOutputs() {
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

static void TestReservedParameter() {
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

static void TestBadPointerReturnsEfault() {
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

static void TestOutputWriteOrderOnPartialFault() {
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

static void TestRoundTripAndTimestamps() {
    constexpr int64_t timestamps[] = {-86400, 0, 1000000, 1767225600, 2147483647};
    for (const int64_t utcIn : timestamps) {
        int64_t local = -1;
        KernelTimesec utcState{-1, 0, 0};
        int32_t utcDst = -1;
        Require(sceKernelConvertUtcToLocaltime(utcIn, &local, &utcState, &utcDst) == 0);
        Require(utcState.t == utcIn);

        int64_t utcOut = -1;
        KernelTimesec localState{-1, 0, 0};
        int32_t localDst = -1;
        Require(sceKernelConvertLocaltimeToUtc(local, 0, &utcOut, &localState, &localDst) == 0);
        Require(utcOut == utcIn);
        Require(localState.t == local);
        Require(localState.west_sec == utcState.west_sec);
        Require(localState.dst_sec == utcState.dst_sec);
    }
}

int main() {
    TestNullOutputs();
    TestReservedParameter();
    TestBadPointerReturnsEfault();
    TestOutputWriteOrderOnPartialFault();
    TestRoundTripAndTimestamps();
    return 0;
}
