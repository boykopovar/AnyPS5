#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_CLOCK_REALTIME = 0;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;

static void Require(bool value) {
    if (!value) std::abort();
}

static void TestClockRealtime() {
    KernelTimespec res{-1, -1};
    Require(clock_getres_nid_postfix(GUEST_CLOCK_REALTIME, &res) == SCE_OK);
    Require(res.tv_sec == 0);
    Require(res.tv_nsec > 0 && res.tv_nsec < NANOS_PER_SECOND);

    KernelTimespec sceRes{-1, -1};
    Require(sceKernelClockGetres(GUEST_CLOCK_REALTIME, &sceRes) == SCE_OK);
    Require(sceRes.tv_sec == res.tv_sec);
    Require(sceRes.tv_nsec == res.tv_nsec);

    KernelTimespec time{-1, -1};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_REALTIME, &time) == SCE_OK);
    Require(time.tv_sec >= 0);
    Require(time.tv_nsec >= 0 && time.tv_nsec < NANOS_PER_SECOND);

    KernelTimespec sceTime{-1, -1};
    Require(sceKernelClockGettime(GUEST_CLOCK_REALTIME, &sceTime) == SCE_OK);
    Require(sceTime.tv_sec >= 0);
    Require(sceTime.tv_nsec >= 0 && sceTime.tv_nsec < NANOS_PER_SECOND);
}

static void TestClockMonotonic() {
    KernelTimespec res{-1, -1};
    Require(clock_getres_nid_postfix(GUEST_CLOCK_MONOTONIC, &res) == SCE_OK);
    Require(res.tv_sec == 0);
    Require(res.tv_nsec > 0 && res.tv_nsec < NANOS_PER_SECOND);

    KernelTimespec sceRes{-1, -1};
    Require(sceKernelClockGetres(GUEST_CLOCK_MONOTONIC, &sceRes) == SCE_OK);
    Require(sceRes.tv_sec == res.tv_sec);
    Require(sceRes.tv_nsec == res.tv_nsec);

    KernelTimespec prev{-1, -1};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &prev) == SCE_OK);
    Require(prev.tv_sec >= 0);
    Require(prev.tv_nsec >= 0 && prev.tv_nsec < NANOS_PER_SECOND);

    for (int i = 0; i < 1000; ++i) {
        KernelTimespec curr{-1, -1};
        Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &curr) == SCE_OK);
        Require(curr.tv_sec > prev.tv_sec || (curr.tv_sec == prev.tv_sec && curr.tv_nsec >= prev.tv_nsec));
        prev = curr;
    }

    KernelTimespec scePrev{-1, -1};
    Require(sceKernelClockGettime(GUEST_CLOCK_MONOTONIC, &scePrev) == SCE_OK);
    Require(scePrev.tv_sec >= 0);
    Require(scePrev.tv_nsec >= 0 && scePrev.tv_nsec < NANOS_PER_SECOND);

    for (int i = 0; i < 1000; ++i) {
        KernelTimespec sceCurr{-1, -1};
        Require(sceKernelClockGettime(GUEST_CLOCK_MONOTONIC, &sceCurr) == SCE_OK);
        Require(sceCurr.tv_sec > scePrev.tv_sec || (sceCurr.tv_sec == scePrev.tv_sec && sceCurr.tv_nsec >= scePrev.tv_nsec));
        scePrev = sceCurr;
    }
}

static void TestInvalidClockIds() {
    for (int clockId : {-1, 9999}) {
        KernelTimespec ts{-1, -1};
        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, &ts) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
        Require(ts.tv_sec == -1 && ts.tv_nsec == -1);

        *__error_nid_postfix() = 0;
        Require(sceKernelClockGettime(static_cast<KernelClockid>(clockId), &ts) == SCE_KERNEL_ERROR_EINVAL);
        Require(ts.tv_sec == -1 && ts.tv_nsec == -1);
    }
}

static void TestNullPointerPassing() {
    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_REALTIME, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    Require(sceKernelClockGettime(GUEST_CLOCK_REALTIME, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGettime(GUEST_CLOCK_MONOTONIC, nullptr) == SCE_KERNEL_ERROR_EFAULT);

    Require(sceKernelClockGetres(GUEST_CLOCK_REALTIME, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGetres(GUEST_CLOCK_MONOTONIC, nullptr) == SCE_KERNEL_ERROR_EFAULT);

    *__error_nid_postfix() = 0;
    Require(clock_getres_nid_postfix(GUEST_CLOCK_REALTIME, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(clock_getres_nid_postfix(GUEST_CLOCK_MONOTONIC, nullptr) == SCE_OK);
    Require(*__error_nid_postfix() == 0);
}

int main() {
    TestClockRealtime();
    TestClockMonotonic();
    TestInvalidClockIds();
    TestNullPointerPassing();
    return 0;
}
