#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

using Nanosleep = int (APS5_VABI *)(const KernelTimespec*, KernelTimespec*);

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EINVAL = 22;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;

static void Require(bool value) {
    if (!value) std::abort();
}

static void TestInvalidNegativeSecAndLargeNanos(Nanosleep sleep, int expectedError) {
    const KernelTimespec request{-1, NANOS_PER_SECOND};
    const KernelTimespec requestNegativeSecLargeNanos{-5, NANOS_PER_SECOND + 500};
    if (sleep == nanosleep_nid_postfix || sleep == _nanosleep_nid_postfix) {
        *__error_nid_postfix() = 0;
        Require(sleep(&request, nullptr) == -1);
        Require(*__error_nid_postfix() == expectedError);
        *__error_nid_postfix() = 0;
        Require(sleep(&requestNegativeSecLargeNanos, nullptr) == -1);
        Require(*__error_nid_postfix() == expectedError);
    } else {
        Require(sleep(&request, nullptr) == expectedError);
        Require(sleep(&requestNegativeSecLargeNanos, nullptr) == expectedError);
    }
}

int main() {
    TestInvalidNegativeSecAndLargeNanos(nanosleep_nid_postfix, GUEST_EINVAL);
    TestInvalidNegativeSecAndLargeNanos(_nanosleep_nid_postfix, GUEST_EINVAL);
    TestInvalidNegativeSecAndLargeNanos(sceKernelNanosleep, SCE_KERNEL_ERROR_EINVAL);
    return 0;
}
