#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;
static constexpr std::int64_t SLEEP_NANOS = 20000000LL;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static void TestNanosleepInvalidNsec() {
    const KernelTimespec boundaryReq{0, NANOS_PER_SECOND};
    const KernelTimespec overReq{0, NANOS_PER_SECOND + 1000LL};
    const KernelTimespec largeReq{1, 2000000000LL};

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&boundaryReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&overReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&largeReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&boundaryReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&overReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sceKernelNanosleep(&boundaryReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&overReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&largeReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec negNsecReq{0, -1LL};
    const KernelTimespec negNsecFarReq{0, -1000000LL};
    const KernelTimespec negBothReq{-1, -1LL};

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
    Require(_nanosleep_nid_postfix(&negNsecReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sceKernelNanosleep(&negNsecReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&negNsecFarReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&negBothReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
}

static void TestNanosleepNullPointer() {
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

    Require(sceKernelNanosleep(nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelNanosleep(nullptr, &remaining) == SCE_KERNEL_ERROR_EFAULT);
}

static void TestClockGettimeInvalidClockId() {
    KernelTimespec ts{};

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(-1, &ts) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(999, &ts) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(0x1000, &ts) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sceKernelClockGettime(static_cast<KernelClockid>(-1), &ts) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelClockGettime(static_cast<KernelClockid>(999), &ts) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelClockGettime(static_cast<KernelClockid>(0x1000), &ts) == SCE_KERNEL_ERROR_EINVAL);
}

static void TestClockGettimeNullPointer() {
    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(0, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(4, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    Require(sceKernelClockGettime(0, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGettime(4, nullptr) == SCE_KERNEL_ERROR_EFAULT);
}

static void TestClockProgression() {
    KernelTimespec realStart{};
    KernelTimespec monoStart{};

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(0, &realStart) == 0);
    Require(*__error_nid_postfix() == 0);
    Require(realStart.tv_sec > 0);
    Require(realStart.tv_nsec >= 0 && realStart.tv_nsec < NANOS_PER_SECOND);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(4, &monoStart) == 0);
    Require(*__error_nid_postfix() == 0);
    Require(monoStart.tv_sec >= 0);
    Require(monoStart.tv_nsec >= 0 && monoStart.tv_nsec < NANOS_PER_SECOND);

    KernelTimespec sceReal{};
    KernelTimespec sceMono{};
    Require(sceKernelClockGettime(0, &sceReal) == SCE_OK);
    Require(sceReal.tv_sec > 0);
    Require(sceReal.tv_nsec >= 0 && sceReal.tv_nsec < NANOS_PER_SECOND);

    Require(sceKernelClockGettime(4, &sceMono) == SCE_OK);
    Require(sceMono.tv_sec >= 0);
    Require(sceMono.tv_nsec >= 0 && sceMono.tv_nsec < NANOS_PER_SECOND);

    const KernelTimespec sleepReq{0, SLEEP_NANOS};
    Require(nanosleep_nid_postfix(&sleepReq, nullptr) == 0);

    KernelTimespec realEnd{};
    KernelTimespec monoEnd{};
    Require(clock_gettime_nid_postfix(0, &realEnd) == 0);
    Require(realEnd.tv_nsec >= 0 && realEnd.tv_nsec < NANOS_PER_SECOND);
    Require(realEnd.tv_sec > realStart.tv_sec ||
            (realEnd.tv_sec == realStart.tv_sec && realEnd.tv_nsec >= realStart.tv_nsec));

    Require(clock_gettime_nid_postfix(4, &monoEnd) == 0);
    Require(monoEnd.tv_nsec >= 0 && monoEnd.tv_nsec < NANOS_PER_SECOND);
    Require(monoEnd.tv_sec > monoStart.tv_sec ||
            (monoEnd.tv_sec == monoStart.tv_sec && monoEnd.tv_nsec >= monoStart.tv_nsec));

    const auto elapsedReal = (realEnd.tv_sec - realStart.tv_sec) * NANOS_PER_SECOND +
                             (realEnd.tv_nsec - realStart.tv_nsec);
    const auto elapsedMono = (monoEnd.tv_sec - monoStart.tv_sec) * NANOS_PER_SECOND +
                             (monoEnd.tv_nsec - monoStart.tv_nsec);
    Require(elapsedReal >= 5000000LL);
    Require(elapsedMono >= 5000000LL);

    KernelTimespec sceSleepReq{0, 10000000LL};
    Require(sceKernelNanosleep(&sceSleepReq, nullptr) == SCE_OK);
}

int main() {
    TestNanosleepInvalidNsec();
    TestNanosleepNullPointer();
    TestClockGettimeInvalidClockId();
    TestClockGettimeNullPointer();
    TestClockProgression();
    return 0;
}
