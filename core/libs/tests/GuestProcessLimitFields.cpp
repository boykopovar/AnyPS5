#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

static void RequireRoundTrip(const GuestResourceLimit& requested, const GuestResourceLimit& expected) {
    constexpr int GUEST_RLIMIT_NPROC = 7;
    constexpr int sentinelError = 13;
    int* const guestError = __error_nid_postfix();
    *guestError = sentinelError;
    const int setResult = setrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &requested);
    if (setResult != 0 || *guestError != sentinelError) {
        std::fprintf(stderr,
            "setrlimit NPROC {cur=%" PRId64 ", max=%" PRId64 "}: result=%d, errno=%d; expected result=0, errno=13\n",
            requested.rlim_cur, requested.rlim_max, setResult, *guestError);
        std::abort();
    }

    GuestResourceLimit actual{};
    *guestError = sentinelError;
    const int getResult = getrlimit_nid_postfix(GUEST_RLIMIT_NPROC, &actual);
    if (getResult != 0 || *guestError != sentinelError ||
        actual.rlim_cur != expected.rlim_cur || actual.rlim_max != expected.rlim_max) {
        std::fprintf(stderr,
            "getrlimit NPROC: result=%d, errno=%d, actual={cur=%" PRId64 ", max=%" PRId64
            "}; expected result=0, errno=13, limit={cur=%" PRId64 ", max=%" PRId64 "}\n",
            getResult, *guestError, actual.rlim_cur, actual.rlim_max, expected.rlim_cur, expected.rlim_max);
        std::abort();
    }
}

int main() {
    RequireRoundTrip(GuestResourceLimit{256, 512}, GuestResourceLimit{256, 512});
    RequireRoundTrip(GuestResourceLimit{0, 512}, GuestResourceLimit{1, 512});
    return 0;
}
